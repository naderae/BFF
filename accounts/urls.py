from . import views
from django.urls import path


app_name = 'accounts'

urlpatterns = [
    path('login/', views.login, name='login'),
    path('logout/', views.logout, name='logout'),
    path('profile/<int:user_id>', views.profile, name='profile'),
    path('profile/<int:user_id>/upload/', views.upload_pics, name='upload_pics'),


]
