from django.db import models
from django.contrib.auth.models import User

# Create your models here.
# the idea here is that one person can only be associacted with one friend object, where he is the owner.
# all of his friends are the friend.users.all()
class Friend(models.Model):
    # this is all the 'owners' friends list
    users = models.ManyToManyField(User)
    # this is the owner of the friends list
    owner = models.ForeignKey(User, related_name='owner', null=True, on_delete=models.CASCADE)


    # we create methods in the models so we can use them in the views!
    # we could have chosen to to create the friends object in the view, but we chose to do it in the model
    # @classmethod
    # def make_friend(cls, current_user, new_friend):
    #     # we create a Friend object called friend
    #     friend, created = cls.objects.get_or_create(
    #     # if the firndship exists, grab it, if not, create one and set the owner as the person logged in
    #     # check if the user that is logged in is the owner of the friends list
    #     # if it doesnt exist, assign the person who is logged in as the owner
    #         current_user = current_user
    #     )
    #     friend.users.add(current_user, new_friend)


    # @classmethod
    # def lose_friend(cls, current_user, new_friend):
    #     friend, created = cls.objects.get_or_create(
    #     # check if the user that is logged in is the owner of the friends list
    #     # if it doesnt exist, assign the person who is logged in as the owner
    #         current_user = current_user
    #     )
    #     friend.users.remove(current_user, new_friend)


    # we add users to this friend object
    # a friend object represents a relationship between two users
    # Friend.users.add(User.objects.first(), User.objects.last())
    # Friend.users.all() will return all the friendships
