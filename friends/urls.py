from . import views
from django.conf.urls import url
from django.urls import path



app_name = 'friends'

urlpatterns = [
    # we have 1 url for both adding and losing a friend
    path('connect/<slug:operation>/<int:pk>/', views.change_friends, name='change_friends'),
    # url(r'^connect/(?P<operation>.+)/(?P<pk>\d+)/$', views.change_friends, name='change_friends')
]
